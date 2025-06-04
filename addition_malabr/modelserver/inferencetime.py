import time
import statistics
from transformers import pipeline

def main():
    print("Loading MobileBERT QA model (PyTorch backend)...")
    qa_pipeline = pipeline(
        "question-answering",
        model="csarron/mobilebert-uncased-squad-v2",
        tokenizer="csarron/mobilebert-uncased-squad-v2",
        framework="pt"
    )
    
    # Define the fixed input (from SQuAD or your test set)
    question = "What day was the game played on?"
    context = ("The game was played on February 7, 2016 at Levi's Stadium "
               "in the San Francisco Bay Area at Santa Clara, California.")
    
    # Warm-up iterations: run these inferences and discard timings.
    warmup_iterations = 100
    print(f"Warming up for {warmup_iterations} iterations...")
    for _ in range(warmup_iterations):
        qa_pipeline({"question": question, "context": context})
    
    # Measurement phase: run a fixed number of iterations and record latency.
    measurement_iterations = 1000  # adjust as needed
    latencies = []
    print(f"Running {measurement_iterations} iterations for measurement...")
    for i in range(measurement_iterations):
        start_time = time.perf_counter()
        qa_pipeline({"question": question, "context": context})
        end_time = time.perf_counter()
        latencies.append((end_time - start_time) * 1000)  # convert seconds to ms
    
    # Compute statistics.
    latencies.sort()
    average_latency = sum(latencies) / len(latencies)
    median_latency = statistics.median(latencies)
    p90_latency = latencies[int(0.9 * len(latencies))]
    
    print("Inference Benchmark Results:")
    print(f"Average latency: {average_latency:.2f} ms")
    print(f"Median latency: {median_latency:.2f} ms")
    print(f"90th percentile latency: {p90_latency:.2f} ms")

if __name__ == "__main__":
    main()

